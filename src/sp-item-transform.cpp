/*
 * Transforming single items
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@gmail.com>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *   Abhishek Sharma
 *   Diederik van Lierop <mail@diedenrezi.nl>
 *
 * Copyright (C) 1999-2011 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <2geom/transforms.h>
#include "sp-item.h"
#include "sp-item-transform.h"

void sp_item_rotate_rel(SPItem *item, Geom::Rotate const &rotation)
{
    Geom::Point center = item->getCenter();
    Geom::Translate const s(item->getCenter());
    Geom::Affine affine = Geom::Affine(s).inverse() * Geom::Affine(rotation) * Geom::Affine(s);

    // Rotate item.
    item->set_i2d_affine(item->i2dt_affine() * (Geom::Affine)affine);
    // Use each item's own transform writer, consistent with sp_selection_apply_affine()
    item->doWriteTransform(item->getRepr(), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
        item->updateRepr();
    }
}

void sp_item_scale_rel(SPItem *item, Geom::Scale const &scale)
{
    Geom::OptRect bbox = item->desktopVisualBounds();
    if (bbox) {
        Geom::Translate const s(bbox->midpoint()); // use getCenter?
        item->set_i2d_affine(item->i2dt_affine() * s.inverse() * scale * s);
        item->doWriteTransform(item->getRepr(), item->transform);
    }
}

void sp_item_skew_rel(SPItem *item, double skewX, double skewY)
{
    Geom::Point center = item->getCenter();
    Geom::Translate const s(item->getCenter());

    Geom::Affine const skew(1, skewY, skewX, 1, 0, 0);
    Geom::Affine affine = Geom::Affine(s).inverse() * skew * Geom::Affine(s);

    item->set_i2d_affine(item->i2dt_affine() * affine);
    item->doWriteTransform(item->getRepr(), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
        item->updateRepr();
    }
}

void sp_item_move_rel(SPItem *item, Geom::Translate const &tr)
{
    item->set_i2d_affine(item->i2dt_affine() * tr);

    item->doWriteTransform(item->getRepr(), item->transform);
}

/**
 * Calculate the affine transformation required to transform one visual bounding box into another, accounting for a uniform strokewidth.
 *
 * PS: This function will only return accurate results for the visual bounding box of a selection of one or more objects, all having
 * the same strokewidth. If the stroke width varies from object to object in this selection, then the function
 * get_scale_transform_for_variable_stroke() should be called instead
 *
 * When scaling or stretching an object using the selector, e.g. by dragging the handles or by entering a value, we will
 * need to calculate the affine transformation for the old dimensions to the new dimensions. When using a geometric bounding
 * box this is very straightforward, but when using a visual bounding box this become more tricky as we need to account for
 * the strokewidth, which is either constant or scales width the area of the object. This function takes care of the calculation
 * of the affine transformation:
 * @param bbox_visual Current visual bounding box
 * @param strokewidth Strokewidth
 * @param transform_stroke If true then the stroke will be scaled proportional to the square root of the area of the geometric bounding box
 * @param x0 Coordinate of the target visual bounding box
 * @param y0 Coordinate of the target visual bounding box
 * @param x1 Coordinate of the target visual bounding box
 * @param y1 Coordinate of the target visual bounding box
 *   PS: we have to pass each coordinate individually, to find out if we are mirroring the object; Using a Geom::Rect() instead is
 *   not possible here because it will only allow for a positive width and height, and therefore cannot mirror
 * @return
 */
Geom::Affine get_scale_transform_for_uniform_stroke(Geom::Rect const &bbox_visual, gdouble strokewidth, bool transform_stroke, gdouble x0, gdouble y0, gdouble x1, gdouble y1)
{
    Geom::Affine p2o = Geom::Translate (-bbox_visual.min());
    Geom::Affine o2n = Geom::Translate (x0, y0);

    Geom::Affine scale = Geom::Scale (1, 1);
    Geom::Affine unbudge = Geom::Translate (0, 0); // moves the object(s) to compensate for the drift caused by stroke width change

    // 1) We start with a visual bounding box (w0, h0) which we want to transfer into another visual bounding box (w1, h1)
    // 2) The stroke is r0, equal for all edges
    // 3) Given this visual bounding box we can calculate the geometric bounding box by subtracting half the stroke from each side;
    // -> The width and height of the geometric bounding box will therefore be (w0 - 2*0.5*r0) and (h0 - 2*0.5*r0)

    gdouble w0 = bbox_visual.width(); // will return a value >= 0, as required further down the road
    gdouble h0 = bbox_visual.height();
    gdouble r0 = fabs(strokewidth);

    // We also know the width and height of the new visual bounding box
    gdouble w1 = x1 - x0; // can have any sign
    gdouble h1 = y1 - y0;
    // The new visual bounding box will have a stroke r1

    // Here starts the calculation you've been waiting for; first do some preparation
    int flip_x = (w1 > 0) ? 1 : -1;
    int flip_y = (h1 > 0) ? 1 : -1;
    
    // w1 and h1 will be negative when mirroring, but if so then e.g. w1-r0 won't make sense
    // Therefore we will use the absolute values from this point on
    w1 = fabs(w1);
    h1 = fabs(h1);
    r0 = fabs(r0);
    // w0 and h0 will always be positive due to the definition of the width() and height() methods.

    // We will now try to calculate the affine transformation required to transform the first visual bounding box into
    // the second one, while accounting for strokewidth

    if ((fabs(w0 - r0) < 1e-6) && (fabs(h0 - r0) < 1e-6)) {
        return Geom::Affine();
    }

    Geom::Affine direct;
    gdouble ratio_x = 1;
    gdouble ratio_y = 1;
    gdouble scale_x = 1;
    gdouble scale_y = 1;
    gdouble r1 = r0;

    if (fabs(w0 - r0) < 1e-6) { // We have a vertical line at hand
        direct = Geom::Scale(flip_x, flip_y * h1 / h0);
        ratio_x = 1;
        ratio_y = (h1 - r0) / (h0 - r0);
        r1 = transform_stroke ? r0 * sqrt(h1/h0) : r0;
        scale_x = 1;
        scale_y = (h1 - r1)/(h0 - r0);
    } else if (fabs(h0 - r0) < 1e-6) { // We have a horizontal line at hand
        direct = Geom::Scale(flip_x * w1 / w0, flip_y);
        ratio_x = (w1 - r0) / (w0 - r0);
        ratio_y = 1;
        r1 = transform_stroke ? r0 * sqrt(w1/w0) : r0;
        scale_x = (w1 - r1)/(w0 - r0);
        scale_y = 1;
    } else { // We have a true 2D object at hand
        direct = Geom::Scale(flip_x * w1 / w0, flip_y* h1 / h0); // Scaling of the visual bounding box
        ratio_x = (w1 - r0) / (w0 - r0); // Only valid when the stroke is kept constant, in which case r1 = r0
        ratio_y = (h1 - r0) / (h0 - r0);
        /* Initial area of the geometric bounding box: A0 = (w0-r0)*(h0-r0)
         * Desired area of the geometric bounding box: A1 = (w1-r1)*(h1-r1)
         * This is how the stroke should scale: r1^2 / A1 = r0^2 / A0
         * So therefore we will need to solve this equation:
         *
         * r1^2 * (w0-r0) * (h1-r1) = r0^2 * (w1-r1) * (h0-r0)
         *
         * This is a quadratic equation in r1, of which the roots can be found using the ABC formula
         * */
        gdouble A = -w0*h0 + r0*(w0 + h0);
        gdouble B = -(w1 + h1) * r0*r0;
        gdouble C = w1 * h1 * r0*r0;
        if (B*B - 4*A*C > 0) {
            // Of the two roots, I verified experimentally that this is the one we need
            r1 = fabs((-B - sqrt(B*B - 4*A*C))/(2*A));
            // If w1 < 0 then the scale will be wrong if we just assume that scale_x = (w1 - r1)/(w0 - r0);
            // Therefore we here need the absolute values of w0, w1, h0, h1, and r0, as taken care of earlier
            scale_x = (w1 - r1)/(w0 - r0);
            scale_y = (h1 - r1)/(h0 - r0);
        } else { // Can't find the roots of the quadratic equation. Likely the input parameters are invalid?
            r1 = r0;
            scale_x = w1 / w0;
            scale_y = h1 / h0;
        }
    }

    // If the stroke is not kept constant however, the scaling of the geometric bbox is more difficult to find
    if (transform_stroke && r0 != 0 && r0 != Geom::infinity()) { // Check if there's stroke, and we need to scale it
        // Now we account for mirroring by flipping if needed
        scale *= Geom::Scale(flip_x * scale_x, flip_y * scale_y);
        // Make sure that the lower-left corner of the visual bounding box stays where it is, even though the stroke width has changed
        unbudge *= Geom::Translate (-flip_x * 0.5 * (r0 * scale_x - r1), -flip_y * 0.5 * (r0 * scale_y - r1));
    } else { // The stroke should not be scaled, or is zero
        if (r0 == 0 || r0 == Geom::infinity() ) { // Strokewidth is zero or infinite
            scale *= direct;
        } else { // Nonscaling strokewidth
            scale *= Geom::Scale(flip_x * ratio_x, flip_y * ratio_y); // Scaling of the geometric bounding box for constant stroke width
            unbudge *= Geom::Translate (flip_x * 0.5 * r0 * (1 - ratio_x), flip_y * 0.5 * r0 * (1 - ratio_y));
        }
    }

    return (p2o * scale * unbudge * o2n);
}

/**
 * Calculate the affine transformation required to transform one visual bounding box into another, accounting for a VARIABLE strokewidth.
 *
 * Note: Please try to understand get_scale_transform_for_uniform_stroke() first, and read all it's comments carefully. This function
 * (get_scale_transform_for_variable_stroke) is a bit different because it will allow for a strokewidth that's different for each
 * side of the visual bounding box. Such a situation will arise when transforming the visual bounding box of a selection of objects,
 * each having a different stroke width. In fact this function is a generalized version of get_scale_transform_for_uniform_stroke(), but
 * will not (yet) replace it because it has not been tested as carefully, and because the old function is can serve as an introduction to
 * understand the new one.
 *
 * When scaling or stretching an object using the selector, e.g. by dragging the handles or by entering a value, we will
 * need to calculate the affine transformation for the old dimensions to the new dimensions. When using a geometric bounding
 * box this is very straightforward, but when using a visual bounding box this become more tricky as we need to account for
 * the strokewidth, which is either constant or scales width the area of the object. This function takes care of the calculation
 * of the affine transformation:
 *
 * @param bbox_visual Current visual bounding box
 * @param bbox_geometric Current geometric bounding box (allows for calculating the strokewidth of each edge)
 * @param transform_stroke If true then the stroke will be scaled proportional to the square root of the area of the geometric bounding box
 * @param x0 Coordinate of the target visual bounding box
 * @param y0 Coordinate of the target visual bounding box
 * @param x1 Coordinate of the target visual bounding box
 * @param y1 Coordinate of the target visual bounding box
 *    PS: we have to pass each coordinate individually, to find out if we are mirroring the object; Using a Geom::Rect() instead is
 *    not possible here because it will only allow for a positive width and height, and therefore cannot mirror
 * @return
 */
Geom::Affine get_scale_transform_for_variable_stroke(Geom::Rect const &bbox_visual, Geom::Rect const &bbox_geom, bool transform_stroke, gdouble x0, gdouble y0, gdouble x1, gdouble y1)
{
    Geom::Affine p2o = Geom::Translate (-bbox_visual.min());
    Geom::Affine o2n = Geom::Translate (x0, y0);

    Geom::Affine scale = Geom::Scale (1, 1);
    Geom::Affine unbudge = Geom::Translate (0, 0);  // moves the object(s) to compensate for the drift caused by stroke width change

    // 1) We start with a visual bounding box (w0, h0) which we want to transfer into another visual bounding box (w1, h1)
    // 2) We will also know the geometric bounding box, which can be used to calculate the strokewidth. The strokewidth will however
    //      be different for each of the four sides (left/right/top/bottom: r0l, r0r, r0t, r0b)

    gdouble w0 = bbox_visual.width(); // will return a value >= 0, as required further down the road
    gdouble h0 = bbox_visual.height();

    // We also know the width and height of the new visual bounding box
    gdouble w1 = x1 - x0; // can have any sign
    gdouble h1 = y1 - y0;
    // The new visual bounding box will have strokes r1l, r1r, r1t, and r1b

    // We will now try to calculate the affine transformation required to transform the first visual bounding box into
    // the second one, while accounting for strokewidth
    gdouble r0w = w0 - bbox_geom.width(); // r0w is the average strokewidth of the left and right edges, i.e. 0.5*(r0l + r0r)
    gdouble r0h = h0 - bbox_geom.height(); // r0h is the average strokewidth of the top and bottom edges, i.e. 0.5*(r0t + r0b)

    int flip_x = (w1 > 0) ? 1 : -1;
    int flip_y = (h1 > 0) ? 1 : -1;

    // w1 and h1 will be negative when mirroring, but if so then e.g. w1-r0 won't make sense
    // Therefore we will use the absolute values from this point on
    w1 = fabs(w1);
    h1 = fabs(h1);
    // w0 and h0 will always be positive due to the definition of the width() and height() methods.

    if ((fabs(w0 - r0w) < 1e-6) && (fabs(h0 - r0h) < 1e-6)) {
        return Geom::Affine();
    }

    Geom::Affine direct;
    gdouble ratio_x = 1;
    gdouble ratio_y = 1;
    gdouble scale_x = 1;
    gdouble scale_y = 1;
    gdouble r1h = r0h;
    gdouble r1w = r0w;

    if (fabs(w0 - r0w) < 1e-6) { // We have a vertical line at hand
        direct = Geom::Scale(flip_x, flip_y * h1 / h0);
        ratio_x = 1;
        ratio_y = (h1 - r0h) / (h0 - r0h);
        r1h = transform_stroke ? r0h * sqrt(h1/h0) : r0h;
        scale_x = 1;
        scale_y = (h1 - r1h)/(h0 - r0h);
    } else if (fabs(h0 - r0h) < 1e-6) { // We have a horizontal line at hand
        direct = Geom::Scale(flip_x * w1 / w0, flip_y);
        ratio_x = (w1 - r0w) / (w0 - r0w);
        ratio_y = 1;
        r1w = transform_stroke ? r0w * sqrt(w1/w0) : r0w;
        scale_x = (w1 - r1w)/(w0 - r0w);
        scale_y = 1;
    } else { // We have a true 2D object at hand
        direct = Geom::Scale(flip_x * w1 / w0, flip_y* h1 / h0); // Scaling of the visual bounding box
        ratio_x = (w1 - r0w) / (w0 - r0w); // Only valid when the stroke is kept constant, in which case r1 = r0
        ratio_y = (h1 - r0h) / (h0 - r0h);
        /* Initial area of the geometric bounding box: A0 = (w0-r0w)*(h0-r0h)
         * Desired area of the geometric bounding box: A1 = (w1-r1w)*(h1-r1h)
         * This is how the stroke should scale:     r1w^2 = A1/A0 * r0w^2, AND
         *                                          r1h^2 = A1/A0 * r0h^2
         * Now we have to solve this set of two equations and find r1w and r1h; this too complicated to do by hand,
         * so I used wxMaxima for that (http://wxmaxima.sourceforge.net/). These lines can be copied into Maxima
         *
         * A1: (w1-r1w)*(h1-r1h);
         * s: A1/A0;
         * expr1a: r1w^2 = s*r0w^2;
         * expr1b: r1h^2 = s*r0h^2;
         * sol: solve([expr1a, expr1b], [r1h, r1w]);
         * sol[1][1]; sol[2][1]; sol[3][1]; sol[4][1];
         * sol[1][2]; sol[2][2]; sol[3][2]; sol[4][2];
         *
         * PS1: The last two lines are only needed for readability of the output, and can be omitted if desired
         * PS2: A0 is known beforehand and assumed to be constant, instead of using A0 = (w0-r0w)*(h0-r0h). This reduces the
         * length of the results significantly
         * PS3: You'll get 8 solutions, 4 for each of the strokewidths r1w and r1h. Some experiments quickly showed which of the solutions
         * lead to meaningful strokewidths
         * */
        gdouble r0h2 = r0h*r0h;
        gdouble r0h3 = r0h2*r0h;
        gdouble r0w2 = r0w*r0w;
        gdouble w12 = w1*w1;
        gdouble h12 = h1*h1;
        gdouble A0 = bbox_geom.area();
        gdouble A02 = A0*A0;

        gdouble operant = 4*h1*w1*A0+r0h2*w12-2*h1*r0h*r0w*w1+h12*r0w2;
        if (operant >= 0) {
            // Of the eight roots, I verified experimentally that these are the two we need
            r1h = fabs((r0h*sqrt(operant)-r0h2*w1-h1*r0h*r0w)/(2*A0-2*r0h*r0w));
            r1w = fabs(-((h1*r0w*A0+r0h2*r0w*w1)*sqrt(operant)+(-3*h1*r0h*r0w*w1-h12*r0w2)*A0-r0h3*r0w*w12+h1*r0h2*r0w2*w1)/((r0h*A0-r0h2*r0w)*sqrt(operant)-2*h1*A02+(3*h1*r0h*r0w-r0h2*w1)*A0+r0h3*r0w*w1-h1*r0h2*r0w2));
            // If w1 < 0 then the scale will be wrong if we just assume that scale_x = (w1 - r1)/(w0 - r0);
            // Therefore we here need the absolute values of w0, w1, h0, h1, and r0, as taken care of earlier
            scale_x = (w1 - r1w)/(w0 - r0w);
            scale_y = (h1 - r1h)/(h0 - r0h);
        } else { // Can't find the roots of the quadratic equation. Likely the input parameters are invalid?
            scale_x = w1 / w0;
            scale_y = h1 / h0;
        }
    }

    // Check whether the stroke is negative; i.e. the geometric bounding box is larger than the visual bounding box, which
    // occurs for example for clipped objects (see launchpad bug #811819)
    if (r0w < 0 || r0h < 0) {
        // How should we handle the stroke width scaling of clipped object? I don't know if we can/should handle this,
        // so for now we simply return the direct scaling
        return (p2o * direct * o2n);
    }

    // The calculation of the new strokewidth will only use the average stroke for each of the dimensions; To find the new stroke for each
    // of the edges individually though, we will use the boundary condition that the ratio of the left/right strokewidth will not change due to the
    // scaling. The same holds for the ratio of the top/bottom strokewidth.
    gdouble stroke_ratio_w = fabs(r0w) < 1e-6 ? 1 : (bbox_geom[Geom::X].min() - bbox_visual[Geom::X].min())/r0w;
    gdouble stroke_ratio_h = fabs(r0h) < 1e-6 ? 1 : (bbox_geom[Geom::Y].min() - bbox_visual[Geom::Y].min())/r0h;

    // If the stroke is not kept constant however, the scaling of the geometric bbox is more difficult to find
    if (transform_stroke && r0w != 0 && r0w != Geom::infinity() && r0h != 0 && r0h != Geom::infinity()) {  // Check if there's stroke, and we need to scale it
        // Now we account for mirroring by flipping if needed
        scale *= Geom::Scale(flip_x * scale_x, flip_y * scale_y);
        // Make sure that the lower-left corner of the visual bounding box stays where it is, even though the stroke width has changed
        unbudge *= Geom::Translate (-flip_x * stroke_ratio_w * (r0w * scale_x - r1w), -flip_y * stroke_ratio_h * (r0h * scale_y - r1h));
    } else { // The stroke should not be scaled, or is zero (or infinite)
        if (r0w == 0 || r0w == Geom::infinity() || r0h == 0 || r0h == Geom::infinity()) { // can't calculate, because apparently strokewidth is zero or infinite
            scale *= direct;
        } else {
            scale *= Geom::Scale(flip_x * ratio_x, flip_y * ratio_y); // Scaling of the geometric bounding box for constant stroke width
            unbudge *= Geom::Translate (flip_x * stroke_ratio_w * r0w * (1 - ratio_x), flip_y * stroke_ratio_h * r0h * (1 - ratio_y));
        }
    }

    return (p2o * scale * unbudge * o2n);
}

Geom::Rect get_visual_bbox(Geom::OptRect const &initial_geom_bbox, Geom::Affine const &abs_affine, gdouble const initial_strokewidth, bool const transform_stroke)
{
    g_assert(initial_geom_bbox);
    
    // Find the new geometric bounding box; Do this by transforming each corner of
    // the initial geometric bounding box individually and fitting a new boundingbox
    // around the transformerd corners  
    Geom::Point const p0 = Geom::Point(initial_geom_bbox->corner(0)) * abs_affine;    
    Geom::Rect new_geom_bbox(p0, p0);
    for (unsigned i = 1 ; i < 4 ; i++) {
        new_geom_bbox.expandTo(Geom::Point(initial_geom_bbox->corner(i)) * abs_affine);
    }

    Geom::Rect new_visual_bbox = new_geom_bbox; 
    if (initial_strokewidth > 0 && initial_strokewidth < Geom::infinity()) {
        if (transform_stroke) {
            // scale stroke by: sqrt (((w1-r0)/(w0-r0))*((h1-r0)/(h0-r0))) (for visual bboxes, see get_scale_transform_for_stroke)
            // equals scaling by: sqrt ((w1/w0)*(h1/h0)) for geometrical bboxes            
            // equals scaling by: sqrt (area1/area0) for geometrical bboxes
            gdouble const new_strokewidth = initial_strokewidth * sqrt (new_geom_bbox.area() / initial_geom_bbox->area());
            new_visual_bbox.expandBy(0.5 * new_strokewidth);        
        } else {
            // Do not transform the stroke
            new_visual_bbox.expandBy(0.5 * initial_strokewidth);   
        }
    }
    
    return new_visual_bbox;
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
